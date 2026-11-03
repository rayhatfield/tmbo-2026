<?php 	
	// Include, and check we've got a connection to the database.
	require_once( 'admin/mysqlConnectionInfo.inc' );
	if(!isset($link) || !$link) $link = openDbConnection();

	require_once( 'offensive/assets/functions.inc' );	
	require_once( 'offensive/assets/tabs.inc' );
	require_once( 'offensive/assets/core.inc' );
	
	function start() {
		mustLogIn();
	}
	

	function searchPager($find, $page, $more) {
		$base = $_SERVER['SCRIPT_NAME']."?c=search&find=".urlencode($find)."&p=";
		?>
		<table width="100%">
			<tr>
				<td width="200px">
					<?php if( $page > 0 ) { ?>
						<a href="<?= htmlEscape($base.($page - 1)) ?>">&laquo;&nbsp;<b>previous&nbsp;page</b></a>
					<?php } ?>
				</td>
				<td></td>
				<td style="text-align:right" width="200px">
					<?php if( $more ) { ?>
						<a href="<?= htmlEscape($base.($page + 1)) ?>"><b>next&nbsp;page</b></a>&nbsp;&raquo;
					<?php } ?>
				</td>
			</tr>
		</table>
		<?php
	}

	function doSearch( $find ) {
		$page = isset($_REQUEST['p']) && is_intger($_REQUEST['p']) ? max(0, (int)$_REQUEST['p']) : 0;
		$numperpage = 25;

		if(comment_search_terms($find) === null) {
			?><div class="entry">Nothing to search for. Words need at least 3 letters, and very common words like "the" are ignored.</div><?php
			return;
		}

		// ask for one extra to find out whether there is a next page
		try {
			$comments = core_searchcomments([
				"q" => $find,
				"limit" => ($page * $numperpage).",".($numperpage + 1),
			]);
		} catch(Exception $e) {
			if($e->getCode() != 413) throw $e;
			?><div class="entry">That search matches too many comments. Add a less common word.</div><?php
			return;
		}
		$more = count($comments) > $numperpage;
		$comments = array_slice($comments, 0, $numperpage);

		?><div class="entry"><?php
		if(count($comments) == 0) {
			echo $page == 0 ? "No results." : "No more results.";
		}
		searchPager($find, $page, $more);
		?></div></div></div><?php

		foreach($comments as $comment) {
			$upload = $comment->upload(); ?>
			<div class="contentbox">
				<div class="heading"><?php
				  // changeblog special header
					if($upload->id() == "211604") { ?>
						<a class="heading" href="<?= Link::content("changeblog") ?>">changeblog</a>
						<br/><span style="color:var(--link-user)">beat to fit. paint to match.</span><br/>
				<?php } else if( $upload->type() == 'topic' ) {
						$prefix = $upload->uploader()->status() == "admin" ? "" : "don't blame me,"; ?>
						<a class="heading" href="<?= Link::thread($upload) ?>"><?= $upload->htmlFilename(); ?></a>
							<br/><span style="color:var(--link-user)"><?= $prefix ?> <a href="<?= Link::user($upload->uploader()) ?>" style="color:var(--link-user)"><?= $upload->uploader()->username() ?></a> started it.</span><br/>
				<?php } else { ?>
						<a class="heading" href="<?= Link::upload($upload) ?>"><?= $upload->htmlFilename(); ?></a><br/><span style="color:var(--link-user)">uploaded by <?= $upload->uploader()->htmlUsername() ?></span><br/>
				<?php } ?>
				</div>
			<div class="entry">
			<?= $comment->HTMLcomment(); ?><br/>

				<div class="timestamp"><a href="<?= Link::comment($comment) ?>"><?= $comment->timestamp() ?></a></div>
				&raquo; <?= $comment->commenter()->htmlUsername() ?>

			<?php

			if( $comment->vote() ) {
				echo "<span class='vote'> [ " . $comment->vote() . " ]</span>";
			}

			if( $comment->tmbo() ) {
				?><span class="vote"> [ this might be offensive ]</span><?php
			}

			if( $comment->tiar() ) {
				?><span class="vote"> [ this is a repost ]</span><?php
			}
			?>
			</div></div>
			<?php
		}

		?>
		<div class="contentbox">
			<div class="entry">
		<?php
		searchPager($find, $page, $more);
	}

	function body() {
?>
<div class="contentbox">
	<div class="blackbar"></div>
	<div class="heading">both hands and a flashlight.</div>
	<?php tabs(); ?>
	<div class="bluebox">

<?php
		$find = trim(array_key_exists("find", $_REQUEST) ? $_REQUEST['find'] : "");

		?>
		<div class="entry">
				<form action="./">
					<input type="hidden" name="c" value="search"/>
					<input type="text" name="find" value="<?= htmlspecialchars($find) ?>"/>
					<input type="submit" value="search comments"/>				
				</form>
		</div>
		<div class="entry">
		<form action="finduser.php">
			<input type="text" name="finduser" value="<?= array_key_exists("finduser", $_REQUEST) ? htmlspecialchars($_REQUEST['finduser']) : "" ?>"/>
			<input type="submit" value="find user"/>
		</form>

		</div>
		<div class="entry">
		<form action="./">
			<input type="text" name="findfile" value="<?= array_key_exists("findfile", $_REQUEST) ? htmlspecialchars($_REQUEST['findfile']) : "" ?>"/>
			<input type="hidden" name="c" value="findfile"/>
			<input type="submit" value="find file or topic"/>
		</form>
		</div>
		<?php
		if( strlen( $find ) > 0 ) {
			doSearch( $find );
		}
		
		
?>
	</div>
	<div class="blackbar"></div>
</div>	
<?php
	}
?>
